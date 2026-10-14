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
Game files must be placed in the games folder, with each game stored in its own directory.

To obtain the files required to play a game:
- Download the APK of a Choice of Games title from one of the following sources:
  https://apkpure.com/developer/Choice%20of%20Games%20LLC
  https://apkpure.com/developer/Hosted%20Games
- Open the APK file with 7-Zip.
- Extract the `assets/mygame` folder.
- Rename the extracted folder to match the game's title.
- Copy the renamed folder into the `games` folder in `CoGSDL`


## Choice script documentation
The documentation of the script can be found [here](https://www.choiceofgames.com/make-your-own-games/choicescript-intro/)