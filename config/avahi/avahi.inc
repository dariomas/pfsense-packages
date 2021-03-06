<?php
/*
	avahi.inc
	part of pfSense (https://www.pfSense.org/)
	Copyright (C) 2009 Scott Ullrich
	Copyright (C) 2009-2013 Jim Pingle
	Copyright (C) 2015 ESF, LLC
	All rights reserved.

	Redistribution and use in source and binary forms, with or without
	modification, are permitted provided that the following conditions are met:

	1. Redistributions of source code must retain the above copyright notice,
	   this list of conditions and the following disclaimer.

	2. Redistributions in binary form must reproduce the above copyright
	   notice, this list of conditions and the following disclaimer in the
	   documentation and/or other materials provided with the distribution.

	THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES,
	INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY
	AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
	AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY,
	OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
	SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
	INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
	CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
	ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
	POSSIBILITY OF SUCH DAMAGE.
*/
$pfs_version = substr(trim(file_get_contents("/etc/version")), 0, 3);
if ($pfs_version == "2.1" || $pfs_version == "2.2") {
	define('AVAHI_BASE', '/usr/pbi/avahi-' . php_uname("m"));
} else {
	define('AVAHI_BASE', '/usr/local');
}

function avahi_install() {
	if (!file_exists('/usr/local/etc/gnome.subr')) {
		@symlink(AVAHI_BASE . '/etc/gnome.subr', '/usr/local/etc/gnome.subr');
	}

	// Add needed users and groups if they don't exist
	if (!exec("/usr/sbin/pw usershow avahi")) {
		exec("/usr/sbin/pw useradd avahi -u 558");
	}
	if (!exec("/usr/sbin/pw groupshow avahi")) {
		exec("/usr/sbin/pw groupadd avahi -g 558");
	}
}

function avahi_deinstall() {
	// Remove created symlink
	unlink_if_exists("/usr/local/etc/gnome.subr");

	// Remove created users and groups if they exist
	if (exec("/usr/sbin/pw groupshow avahi")) {
		exec("/usr/sbin/pw groupdel avahi");
	}
	if (exec("/usr/sbin/pw usershow avahi")) {
		exec("/usr/sbin/pw userdel avahi");
	}
}

function avahi_write_config() {
	global $config, $avahi_config;
	conf_mount_rw();

	// Pull some various values out of config.xml
	if (isset($config['installedpackages']['avahi']['config'][0])) {
		$avahi_config = $config['installedpackages']['avahi']['config'][0];
	}
	// Server Options
	$hostname = $config['system']['hostname'];
	$domain = $config['system']['domain'];
	$enable = $avahi_config['enable'];
	$browsedomains = $avahi_config['browsedomains'] ?: "local";
	$denyif = $avahi_config['denyinterfaces'];
	$useipv4 = ($avahi_config['enable_ipv4']) ? "yes" : "no";
	$useipv6 = ($avahi_config['enable_ipv6']) ? "yes" : "no";
	$usedbus = ($avahi_config['disable_dbus']) ? "no" : "yes";
	// Wide Area
	$widearea = ($avahi_config['enable_wide_area']) ? "yes" : "no";
	// Publishing Options
	$publish = ($avahi_config['disable_publishing']) ? "no" : "yes";
	$userpublish = ($avahi_config['disable_user_service_publishing']) ? "no" : "yes";
	$addresspublish = ($avahi_config['publish_addresses']) ? "yes" : "no";
	$cookie = ($avahi_config['add_service_cookie']) ? "yes" : "no";
	$hinfopublish = ($avahi_config['publish_hinfo']) ? "yes" : "no";
	$wspublish = ($avahi_config['publish_workstation']) ? "yes" : "no";
	$aaaaonv4 = ($avahi_config['publish_aaaa_on_ipv4']) ? "yes" : "no";
	$aonv6 = ($avahi_config['publish_a_on_ipv6']) ? "yes" : "no";
	// Reflector Options
	$reflect = ($avahi_config['enable_reflector']) ? "yes" : "no";
	$reflectipv = ($avahi_config['reflect_ipv']) ? "yes" : "no";

	// Never pass along WAN. Bad.
	$denyinterfaces = $config['interfaces']['wan']['if'];

	// Process interfaces defined by user to deny.
	if ($denyif) {
		$if = explode(",", $denyif);
		foreach ($if as $i) {
			$ifreal = convert_friendly_interface_to_real_interface_name($i);
			if ($ifreal) {
				$denyinterfaces .= ", " . $ifreal;
			}
		}
	}
	
	// Process DNS servers and omit localhost if present in /etc/resolv.conf
	$publishdns = '';
	$dns = implode(" ", get_dns_servers());
	$dns = trim(str_replace('127.0.0.1', '', $dns));
	$dns = str_replace(' ', ', ', $dns);
	if ($dns) {
		$publishdns = "publish-dns-servers={$dns}";
	}

	// Construct the avahi configuration
	$avahiconfig = <<<EOF

# avahi.conf - This file was automatically generated by the pfSense pacakge
# manager. Do not edit this file, it will be overwritten automatically.
# See /usr/local/pkg/avahi.inc to make changes to this file!

[server]
host-name={$hostname}
domain-name=local
browse-domains={$browsedomains}
deny-interfaces={$denyinterfaces}
use-ipv4={$useipv4}
use-ipv6={$useipv6}
enable-dbus={$usedbus}
#check-response-ttl=no
#use-iff-running=no
#disallow-other-stacks=no
allow-point-to-point=yes

[wide-area]
enable-wide-area={$widearea}

[publish]
disable-publishing={$publish}
disable-user-service-publishing={$userpublish}
add-service-cookie={$cookie}
publish-addresses={$addresspublish}
publish-hinfo={$hinfopublish}
publish-workstation={$wspublish}
#publish-domain=yes
{$publishdns}
#publish-resolv-conf-dns-servers=yes
publish-aaaa-on-ipv4={$aaaaonv4}
publish-a-on-ipv6={$aonv6}

[reflector]
enable-reflector={$reflect}
reflect-ipv={$reflectipv}

[rlimits]
rlimit-core=0
rlimit-data=4194304
rlimit-fsize=0
rlimit-nofile=300
rlimit-stack=4194304
rlimit-nproc=3
#rlimit-as=

EOF;

	/* Write out .conf file */
	safe_mkdir(AVAHI_BASE . "/etc/avahi");
	$fd = fopen(AVAHI_BASE . "/etc/avahi/avahi-daemon.conf", "w");
	fwrite($fd, $avahiconfig);
	fclose($fd);

	/* Write out rc.d startup file */
	$start = "/etc/rc.conf_mount_rw\n";
	$start .= "if [ ! -d /proc/0 ]; then\n";
	$start .= "	/bin/mkdir -p /proc\n";
	$start .= "	/sbin/mount -t procfs procfs /proc\n";
	$start .= "fi\n";
	$start .= "if [ ! -f /usr/local/etc/gnome.subr ]; then\n";
	$start .= "	/bin/ln -sf " . AVAHI_BASE . "/etc/gnome.subr /usr/local/etc/gnome.subr\n";
	$start .= "fi\n";
	$start .= "/usr/bin/killall avahi-daemon >/dev/null 2>&1\n";
	if (!$avahi_config['disable_dbus']) {
		$start .= "if [ ! -d /var/run/dbus ]; then\n";
		$start .= "        /bin/mkdir /var/run/dbus\n";
		$start .= "        /usr/sbin/chown messagebus:messagebus /var/run/dbus\n";
		$start .= "fi\n";
			if (file_exists(AVAHI_BASE . "/etc/rc.d/dbus")) {
				$start .= AVAHI_BASE . "/etc/rc.d/dbus onestop\n";
				$start .= "/bin/rm /var/run/dbus/dbus.pid >/dev/null 2>&1\n";
				$start .= AVAHI_BASE . "/etc/rc.d/dbus onestart\n";
				$start .= "sleep 5\n";
			}
	}
	$start .= AVAHI_BASE . "/sbin/avahi-daemon -D\n";
	$start .= "/etc/rc.conf_mount_ro\n";

	$stop = "/usr/bin/killall avahi-daemon >/dev/null 2>&1\n";
	if (!$avahi_config['disable_dbus']) {
		if (file_exists(AVAHI_BASE . "/etc/rc.d/dbus")) {
			$stop .= AVAHI_BASE . "/etc/rc.d/dbus onestop\n";
			$stop .= "/bin/rm /var/run/dbus/dbus.pid >/dev/null 2>&1\n";
		}
	}

	write_rcfile(array(
		"file" => "avahi-daemon.sh",
		"start" => $start,
		"stop" => $stop
		)
	);

	conf_mount_ro();
}

function avahi_sync() {
	global $config;

	if (is_service_running("avahi")) {
		stop_service("avahi");
	}
	avahi_write_config();
	// Is package enabled?
	if ($config['installedpackages']['avahi']['config'][0]['enable']) {
		start_service("avahi");
	}
}

function avahi_upgrade_config() {
	global $config, $avahi_config;

	$avahi_config =& $config['installedpackages']['avahi']['config'][0];
	if (!is_array($avahi_config)) {
		$avahi_config = array();
	}
	$changes = 0;
	// Convert previous disable values to enable ones
	// If broken settings were configured due to lack of validation, just set to defaults
	if (isset($avahi_config['disable_ipv4']) && isset($avahi_config['disable_ipv6']) && $avahi_config['disable_ipv4'] == "on" && $avahi_config['disable_ipv6'] == "on") {
		$avahi_config['enable_ipv4'] = "on";
		$avahi_config['enable_ipv6'] = "on";
		unset($avahi_config['disable_ipv4']);
		unset($avahi_config['disable_ipv6']);
		$changes++;
	} elseif (isset($avahi_config['disable_ipv4']) && $avahi_config['disable_ipv4'] != "on") {
		$avahi_config['enable_ipv4'] = "on";
		unset($avahi_config['disable_ipv4']);
		$changes++;
	} elseif (isset($avahi_config['disable_ipv6']) && $avahi_config['disable_ipv6'] != "on") {
		$avahi_config['enable_ipv6'] = "on";
		unset($avahi_config['disable_ipv6']);
		$changes++;
	}
	if ($changes > 0 ) {
		write_config("[avahi] Upgraded old package configuration.");
	}
}

function avahi_validate_input($post, &$input_errors) {
	if (($post['enable_ipv4'] != "on" ) && ($post['enable_ipv6'] != "on" )) {
		$input_errors[] = gettext("You cannot disable both IPv4 and IPv6!");
	}
	if (($post['enable_ipv4'] != "on" ) && ($post['publish_aaaa_on_ipv4'] == "on" )) {
		$input_errors[] = gettext("'Publish AAAA records on IPv4' makes no sense with IPv4 disabled.");
	}
	if (($post['enable_ipv6'] != "on" ) && ($post['publish_a_on_ipv6'] == "on" )) {
		$input_errors[] = gettext("'Publish A records on IPv6' makes no sense with IPv6 disabled.");
	}
}

?>
