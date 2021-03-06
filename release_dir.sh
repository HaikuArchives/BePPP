#!/bin/sh
echo Making Release Directory...
mkdir PPPoE
echo Creating Directory Structure...
mkdir PPPoE/src PPPoE/bin PPPoE/bin/x86 PPPoE/bin/ppc
echo Copying source files...
cp -r $1/common PPPoE/src
cp -r $1/pppoe PPPoE/src
echo Copying documentation...
cp -r $1/documentation PPPoE/Documentation
echo Placing PPC binaries...
mv PPPoE/src/pppoe/obj.ppc/pppoe PPPoE/bin/ppc
mv PPPoE/src/pppoe/obj.ppc/pppoe.xMAP PPPoE/bin/ppc
echo Placing x86 binaries...
mv PPPoE/src/pppoe/obj.x86/pppoe PPPoE/bin/x86
echo Removing CVS and obj Directories...
rm -rf PPPoE/src/*/CVS PPPoE/src/*/obj.* PPPoE/Documentation/CVS
echo Fixing MIME types lost during CVS
mimeset -all -f PPPoE
echo All Done