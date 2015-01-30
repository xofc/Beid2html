# Beid2html
A small standalone program reading Belgian eid cards on USB/CCID devices using just libusb 1.0 on Linux and creating an HTML file (with a JPG picture).

This program is a revision of the one developed in 2007 in http://myacr38.blogspot.be/.
It now uses libusb 1.0 and USB/CCID class devices (tested on a Digipass-870 by http://Vasco.com)
It is based on the 2007 blog experiments and
  - http://www.usb.org/developers/docs/devclass_docs/DWG_Smart-Card_CCID_Rev110.pdf
  - http://libusb.sourceforge.net/api-1.0/

There are still some 'error messages' at the beginning...
I am not sure where it comes from, it seems that something needs time to come up (?)
I could hide them but it isn't harmful.

To work,
  - the card reader has to be freed (i.e. run 'sudo /etc/init.d/pcscd stop' if necessary)
  - you should have the permission to interact with the USB device
          (either run the command with 'sudo' or chmod the usb device)

The program does not use arguments and produces two files :
   - &lt;NationalRegisterNumber>.html
   - &lt;NationalRegisterNumber>.jpg
(note that is is unlawful to use the &lt;NationalRegisterNumber> for any purpose without authorization)
