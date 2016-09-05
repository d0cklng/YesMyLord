#-------------------------------------------------
# Project subs & dirs setting. target for all
# Create at 2013/02/17
# Create by Timelink Inc.
#-------------------------------------------------

TEMPLATE = subdirs

SUBDIRS = \
    AutoTest \
    xplow \
    xlors \
    xlorb \
    xlord \
    #xdht \   #dht server
    base \
    crypto \
    module \
    miniupnpc \
    jinlord \
    #jinchart \
    jinraknet

!win32 {
SUBDIRS += libev
module.depends = libev
}

## dir--pro
xlorb.file=./xlors/xlorb.pro
xlord.file=./xlors/xlord.pro

## depends
#miniupnpc.depends = base
module.depends = miniupnpc
jinraknet.depends = module
jinlord.depends = jinraknet module base crypto jinchart miniupnpc
AutoTest.depends = base crypto module jinchart
xplow.depends = base module
xlors.depends = base module jinraknet
xlord.depends = base module jinraknet
xlorb.depends = base module jinraknet
xplow.depends = base module
xdht.depends = base module jinchart
#depends

