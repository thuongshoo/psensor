#!/bin/bash
set -euo pipefail

# Create debian directory structure
mkdir -p debian/source

# Create debian/source/format
echo "3.0 (quilt)" > debian/source/format

# Create debian/compat to set debhelper compatibility level. We avoid
# depending on debhelper-compat in debian/control to prevent the
# compatibility level from being specified twice which can cause dh to
# fail in some environments.
echo "13" > debian/compat

# Create debian/control
cat > debian/control << 'EOF'
Source: psensor
Section: utils
Priority: optional
Maintainer: thuongshoo <yuyoonshoo@gmail.com>
Build-Depends: debhelper (>= 13),
               cmake,
               pkg-config,
               libgtk-3-dev (>= 3.4),
               libappindicator3-dev,
               libsensors-dev,
               libnotify-dev,
               libglib2.0-dev,
               libjson-c-dev,
               libcurl4-openssl-dev,
               libmicrohttpd-dev,
               libatasmart-dev,
               libunity-dev,
               help2man,
               gettext,
               asciidoc

Package: psensor
Architecture: any
Depends: ${shlibs:Depends},
         ${misc:Depends}
Description: Hardware temperature monitor
 Psensor is a graphical hardware temperature monitor for Linux.
 .
 It can monitor:
  * the temperature of the motherboard and CPU sensors
  * the temperature of the NVidia GPUs
  * the temperature of the Hard Disk Drives
  * the rotation speed of the fans
  * the CPU usage
EOF

# Create debian/rules với fix cho file .mo
cat > debian/rules << 'EOF'
#!/usr/bin/make -f
%:
	dh $@ --buildsystem=cmake

override_dh_auto_configure:
	dh_auto_configure -- -DCMAKE_INSTALL_PREFIX=/usr -DCMAKE_BUILD_TYPE=Release

# NGĂN KHÔNG CHO NORMALIZE FILE .mo
override_dh_strip_nondeterminism:
	dh_strip_nondeterminism -X.mo
EOF

# Create debian/copyright
cat > debian/copyright << 'EOF'
Format: https://www.debian.org/doc/packaging-manuals/copyright-format/1.0/
Upstream-Name: psensor
Source: https://github.com/thuongshoo/psensor

Files: *
Copyright: 2023-2025 thuongshoo <yuyoonshoo@gmail.com>
License: GPL-2+
 This package is free software; you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation; either version 2 of the License, or
 (at your option) any later version.
 .
 This package is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.
 .
 You should have received a copy of the GNU General Public License
 along with this program. If not, see <https://www.gnu.org/licenses/>
 .
 On Debian systems, the complete text of the GNU General
 Public License version 2 can be found in "/usr/share/common-licenses/GPL-2".
EOF

# Create debian/changelog
cat > debian/changelog << 'EOF'
psensor (1.2.0-1) unstable; urgency=medium

  * Initial release.

 -- thuongshoo <yuyoonshoo@gmail.com>  Fri, 08 Nov 2025 17:00:00 +0000
EOF

# Make rules executable
chmod +x debian/rules

# Build the package
dpkg-buildpackage -b -us -uc