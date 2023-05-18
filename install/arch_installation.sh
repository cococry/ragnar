#/bin/bash

sudo pacman -S xorg xorg-dev xorg-xinit gcc make git freetype2
git clone https://github.com/cococry/Ragnar.git 

cd ~/Ragnar
sudo make ragnar install

if [ ! -f ~/.xinitrc ] 
then
    touch ~/.xinitrc
    echo "exec ragnar" > ~/.xinitrc
fi
