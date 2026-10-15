# CoGSDL

## Install on the Miyoo Mini
To install CoGSDL on the Miyoo Mini:
- Download the latest [release](https://github.com/sakya/CoGSDL/releases) ZIP file
- Extract the contents of the ZIP file into the `/App/CoGSDL` directory on the SD card.

Make sure the files are extracted directly into `/App/CoGSDL`, rather than into an additional nested directory.

## Installing games
Game files must be placed in the `/App/CoGSDL/games` folder, with each game stored in its own directory.

To obtain the files required to play a game:
- Download the APK of a ChoiceScript title from one of the following sources:  
  https://apkpure.com/developer/Choice%20of%20Games%20LLC  
  https://apkpure.com/developer/Hosted%20Games
- Open the APK file with 7-Zip.
- Extract the `assets/mygame` folder.
- Rename the `mygame` folder to match the game's title.
- Copy the renamed folder into the `/App/CoGSDL/games` folder

## Screenshots
<img width="640" height="480" alt="CoGSDL_001" src="https://github.com/user-attachments/assets/be329c87-3d81-48b0-9d20-a2707c9797ea" />

<img width="640" height="480" alt="CoGSDL_002" src="https://github.com/user-attachments/assets/bdb3abc9-eb81-42a8-ad97-b2c9875dfe22" />

<img width="640" height="480" alt="CoGSDL_000" src="https://github.com/user-attachments/assets/18a8efdb-c0d1-4789-8906-fd7cfd476676" />

<img width="640" height="480" alt="CoGSDL_003" src="https://github.com/user-attachments/assets/6d8284b3-905e-4f5e-a929-8c8305086daa" />

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

## ChoiceScript documentation
The documentation of the script can be found [here](https://www.choiceofgames.com/make-your-own-games/choicescript-intro/)
