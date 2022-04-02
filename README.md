### Rivendell Radio Automation for ARM Hardware

Welcome to the github for the Rivendell on ARM project. This release has no affiliation with the official Rivendell software. We aim to provide a stable release for those who wish to use the software on ARM based hardware, such as the Raspberry Pi, Pinebook Pro, Orange Pi, ASUS Tinker Board and other single board computers. We will be merging as much of the official code as possible. Most of the changes in this repository are about running Rivendell reliably on ARM based hardware using Debian 10 Buster. The packages are provided as is and come with no guarentee or warranty.

#### Please see the [wiki](https://github.com/edgeradio993fm/rivendell/wiki) page for the latest install guides and updates.

***
### Tested Hardware
Raspberry Pi 3B using Raspberry Pi OS legacy<br>
Raspberry Pi 3B+ using Raspberry Pi OS legacy<br>
Raspberry Pi 4 Model B using Raspberry Pi OS legacy<br>
Raspberry Pi 400 using Raspberry Pi OS legacy<br>
ASUS Tinker Board using Armbian Buster<br>
Pine64 Pinebook Pro using Armbian Buster<br>
Pine64 ROCKPro64 using Armbian Buster<p>

*Please Note: There is no support for Audio Science sound cards on ARM based hardware. ARM SoC sound cards do not perform well in most cases. We recommend using USB audio cards for best compatibility.*

***
### Latest Packages Avaliable
##### ARMHF
[![Latest version of 'rivendell' @ Cloudsmith](https://api-prd.cloudsmith.io/v1/badges/version/7edg/rivendell-arm/deb/rivendell/latest/a=armhf;d=debian%252Fany-version;t=binary/?render=true&show_latest=true)](https://cloudsmith.io/~7edg/repos/rivendell-arm/packages/detail/deb/rivendell/latest/a=armhf;d=debian%252Fany-version;t=binary/)
##### ARM64
[![Latest version of 'rivendell' @ Cloudsmith](https://api-prd.cloudsmith.io/v1/badges/version/7edg/rivendell-arm/deb/rivendell/latest/a=arm64;d=debian%252Fany-version;t=binary/?render=true&show_latest=true)](https://cloudsmith.io/~7edg/repos/rivendell-arm/packages/detail/deb/rivendell/latest/a=arm64;d=debian%252Fany-version;t=binary/)
##### AMD64
[![Latest version of 'rivendell' @ Cloudsmith](https://api-prd.cloudsmith.io/v1/badges/version/7edg/rivendell-arm/deb/rivendell/latest/a=amd64;d=debian%252Fany-version;t=binary/?render=true&show_latest=true)](https://cloudsmith.io/~7edg/repos/rivendell-arm/packages/detail/deb/rivendell/latest/a=amd64;d=debian%252Fany-version;t=binary/)

***

### This is the README for the Rivendell package.

This software comes with ABSOLUTELY NO WARRANTY. See the file COPYING for
details.

Welcome to this release of the Rivendell Project. Rivendell aims
to be a robust, functionally complete digital audio system for use in 
professional radio broadcast environments.

See the file INSTALL for installation instructions. If you are upgrading from
a 1.x or 2.x version of Rivendell, also see the file 'UPGRADING'
for important notes and pointers.


OVERVIEW
Rivendell contains a full set of functionality needed to operate a radio
automation system, consisting of the following components:

RDAdmin
A comprehensive application for the administration and configuration
of Rivendell.

RDLibrary
The production audio interface.

RDCatch
The automatic recorder and task scheduler interface.

RDLogin
A small utility for logging users into and out of the system.

RDLogEdit
A air log creation, editing and voicetracker tool.

RDLogManager
A utility for the automatic generation of logs, based on templates.
It also supports importation of schedule information from a wide
variety of third-party traffic and music scheduling systems.

RDAirPlay
The on-air playout application.

RDPanel
A 'fullscreen' SoundPanel cartwall application.

RDCartSlots
An application that emulates a multi-slot cart decks.

RDCastManager
A utility for posting and managing podcast episodes.

RDGpiMon
A utility for monitoring GPI line status.

RDAlsaConfig
A utility for configuring ALSA audio adapters.

RDDbConfig
A utility for creating, backing up and restoring the Rivendell
database.

RDImport
A command-line utility for batch importing audio along with
accompanying metadata. Do:

	man 1 rdimport

for usage help.

RMLSend
A GUI and command-line utility for sending Rivendell Macro Language
(RML) commands.

RDSoftKeys
A lightweight button panel utility for sending RML commands.
