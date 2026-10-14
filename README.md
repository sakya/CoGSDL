# CoGSDL

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
- Create the package
  ```bash
  make pack
  ```

## Installing games
Games files must be put in the `games` folder.
Each game should be in its directory.

https://apkpure.com/developer/Choice%20of%20Games%20LLC
https://apkpure.com/developer/Hosted%20Games

## Choice script documentation
The documentation of the script can be found [here](https://www.choiceofgames.com/make-your-own-games/choicescript-intro/)