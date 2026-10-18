# CoGSDL
Unofficial [ChoiceScript](https://www.choiceofgames.com/make-your-own-games/choicescript-intro/) interpreter, built with SDL2.

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
<img width="640" height="480" alt="CoGSDL_000" src="https://github.com/user-attachments/assets/b8705787-073f-4229-8329-528ea7f487ed" />

<img width="640" height="480" alt="CoGSDL_001" src="https://github.com/user-attachments/assets/ef96cb4d-9ce3-4d74-9b08-907c4d0d8b57" />

<img width="640" height="480" alt="CoGSDL_002" src="https://github.com/user-attachments/assets/242469a1-4c7e-4568-bce3-2d038426a0df" />

<img width="640" height="480" alt="CoGSDL_003" src="https://github.com/user-attachments/assets/3ffe8903-54bd-45b9-b4c5-41ee7a3111d7" />

<img width="640" height="480" alt="CoGSDL_004" src="https://github.com/user-attachments/assets/5f65ab8f-2818-4503-ae4f-a260fbae243c" />

<img width="640" height="480" alt="CoGSDL_005" src="https://github.com/user-attachments/assets/f53f9b0e-41f1-4763-83c4-46e3d2c473aa" />

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
