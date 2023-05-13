sudo apt-get install xorg xorg-dev gcc make git
sudo apt-get install libfreetype6-dev
git clone https://github.com/suleyman-kaya/Ragnar.git
cd Ragnar
sudo make clean ubuntu_install
sudo cp ragnar.desktop /usr/share/xsessions/ragnar.desktop
