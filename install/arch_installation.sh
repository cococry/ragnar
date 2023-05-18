#/bin/bash

sudo pacman -S xorg xorg-dev xorg-xinit gcc make git freetype2
git clone https://github.com/cococry/Ragnar.git 


echo "! CLONED AT '$(cd)'"
cd ~/Ragnar
sudo make ragnar install
echo "! INSTALLED"

if [ ! -f ~/.xinitrc ] 
then
    touch ~/.xinitrc
    echo "exec ragnar" > ~/.xinitrc
    echo "! WRITTEN TO ~/.xinitrc"
fi
