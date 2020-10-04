Sample init scripts and service configuration for axed
==========================================================

Sample scripts and configuration files for systemd, Upstart and OpenRC
can be found in the contrib/init folder.

    contrib/init/axed.service:    systemd service unit configuration
    contrib/init/axed.openrc:     OpenRC compatible SysV style init script
    contrib/init/axed.openrcconf: OpenRC conf.d file
    contrib/init/axed.conf:       Upstart service configuration file
    contrib/init/axed.init:       CentOS compatible SysV style init script

Service User
---------------------------------

All three Linux startup configurations assume the existence of a "axecore" user
and group.  They must be created before attempting to use these scripts.
The OS X configuration assumes axed will be set up for the current user.

Configuration
---------------------------------

At a bare minimum, axed requires that the rpcpassword setting be set
when running as a daemon.  If the configuration file does not exist or this
setting is not set, axed will shutdown promptly after startup.

This password does not have to be remembered or typed as it is mostly used
as a fixed token that axed and client programs read from the configuration
file, however it is recommended that a strong and secure password be used
as this password is security critical to securing the wallet should the
wallet be enabled.

If axed is run with the "-server" flag (set by default), and no rpcpassword is set,
it will use a special cookie file for authentication. The cookie is generated with random
content when the daemon starts, and deleted when it exits. Read access to this file
controls who can access it through RPC.

By default the cookie is stored in the data directory, but it's location can be overridden
with the option '-rpccookiefile'.

This allows for running axed without having to do any manual configuration.

`conf`, `pid`, and `wallet` accept relative paths which are interpreted as
relative to the data directory. `wallet` *only* supports relative paths.

For an example configuration file that describes the configuration settings,
see `contrib/debian/examples/axe.conf`.

Paths
---------------------------------

### Linux

All three configurations assume several paths that might need to be adjusted.

Binary:              `/usr/bin/axed`
Configuration file:  `/etc/axecore/axe.conf`
Data directory:      `/var/lib/axed`  
PID file:            `/var/run/axed/axed.pid` (OpenRC and Upstart) or `/var/lib/axed/axed.pid` (systemd)
Lock file:           `/var/lock/subsys/axed` (CentOS)

The configuration file, PID directory (if applicable) and data directory
should all be owned by the axecore user and group.  It is advised for security
reasons to make the configuration file and data directory only readable by the
axecore user and group.  Access to axe-cli and other axed rpc clients
can then be controlled by group membership.

### Mac OS X

Binary:              `/usr/local/bin/axed`
Configuration file:  `~/Library/Application Support/AxeCore/axe.conf`
Data directory:      `~/Library/Application Support/AxeCore`
Lock file:           `~/Library/Application Support/AxeCore/.lock`

Installing Service Configuration
-----------------------------------

### systemd

Installing this .service file consists of just copying it to
/usr/lib/systemd/system directory, followed by the command
`systemctl daemon-reload` in order to update running systemd configuration.

To test, run `systemctl start axed` and to enable for system startup run
`systemctl enable axed`

### OpenRC

Rename axed.openrc to axed and drop it in /etc/init.d.  Double
check ownership and permissions and make it executable.  Test it with
`/etc/init.d/axed start` and configure it to run on startup with
`rc-update add axed`

### Upstart (for Debian/Ubuntu based distributions)

Drop axed.conf in /etc/init.  Test by running `service axed start`
it will automatically start on reboot.

NOTE: This script is incompatible with CentOS 5 and Amazon Linux 2014 as they
use old versions of Upstart and do not supply the start-stop-daemon utility.

### CentOS

Copy axed.init to /etc/init.d/axed. Test by running `service axed start`.

Using this script, you can adjust the path and flags to the axed program by
setting the AXED and FLAGS environment variables in the file
/etc/sysconfig/axed. You can also use the DAEMONOPTS environment variable here.

### Mac OS X

Copy org.axe.axed.plist into ~/Library/LaunchAgents. Load the launch agent by
running `launchctl load ~/Library/LaunchAgents/org.axe.axed.plist`.

This Launch Agent will cause axed to start whenever the user logs in.

NOTE: This approach is intended for those wanting to run axed as the current user.
You will need to modify org.axe.axed.plist if you intend to use it as a
Launch Daemon with a dedicated axecore user.

Auto-respawn
-----------------------------------

Auto respawning is currently only configured for Upstart and systemd.
Reasonable defaults have been chosen but YMMV.
