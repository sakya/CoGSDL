# CoGSDL

https://www.choiceofgames.com/make-your-own-games/choicescript-intro/


https://apkpure.com/developer/Choice%20of%20Games%20LLC
https://apkpure.com/developer/Hosted%20Games

##Toolchain
https://onionui.github.io/docs/dev/setup

## Build for Miyoo Mini
- Install the toolchain
  ```bash
  git clone -c core.autocrlf=false https://github.com/shauninman/union-miyoomini-toolchain.git
  cd union-miyoomini-toolchain
  make shell
  ```
- Clone CoGSDL
  ```bash
  git clone https://github.com/sakya/CoGSDL.git
  ```
- Build sdl2
  ```bash
  cd CoGSDL
  apt-get update && apt-get install -y autoconf automake libtool pkg-config
  sh miyoo/build-sdl2.sh
  ```
- Build CoGSDL
  ```bash
  make re PLATFORM=miyoo
  ```