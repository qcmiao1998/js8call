Some versions of Mac OS need special system settings to allow a shared memory
segment to be created (this is how the application communicates with the decoder).

Included in this DMG is a sysctl.conf file that you can use to increase the 
shared memory that the operating system allows to be allocated. You only have to do this process once. 

To install:

0) Caution! Read through these instructions completely before proceeding. 
   If you are at all uncomfortable modifying your system, please feel free to 
   email me and I will help you walk though steps: kn4crd@gmail.com

1) Copy the sysctl.conf file to your desktop

2) Open a Terminal window (Applications -> Utilities -> Terminal.app)

3) Run these commands in the Terminal window to install the sysctl.conf

    sudo cp $HOME/Desktop/sysctl.conf /etc/sysctl.conf
    sudo chown root:wheel /etc/sysctl.conf
    sudo chmod 0664 /etc/sysctl.conf

4) Reboot your system. 

If you run into any problems, feel free to reach out to me and I will help you
walk through the steps: Jordan Sherer <kn4crd@gmail.com>
