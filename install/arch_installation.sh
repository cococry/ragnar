#/bin/bash

sudo pacman -S --needed xorg xorg-xinit gcc make git freetype2
git clone https://github.com/cococry/Ragnar.git 
cd ~/Ragnar
sudo cp ragnar.desktop /usr/share/xsessions/ragnar.desktop
sudo make ragnar install
echo "! INSTALLED"

if [ ! -f ~/.xinitrc ] 
then
    touch ~/.xinitrc
    echo "exec ragnar" > ~/.xinitrc
    echo "! WRITTEN TO ~/.xinitrc"
fi
