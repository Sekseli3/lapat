# Tehtävä

## Riippuvuudet
Ohjelma käyttää OpenCV4, protobuf, abseil ja pkg-config kirjastoja
Lataukset:

brew install opencv

brew install protobuf

brew install abseil

brew install pkg-config

## Buildaus
Aluksi pitää generoida protomallista C koodi komennolla
```bash
protoc --cpp_out=. Rectangle.proto
```

 ### Muuta main.cpp alussa oleva VIDEO_PATH oman videon sijainnin mukaan

Buildaus tapahtuu pkg-config käyttämällä ja g++.

buildaus:
```bash
g++ -O3 `pkg-config --cflags --libs opencv4 protobuf` -std=c++17 main.cpp Rectangle.pb.cc -o main
```

Runaus:
```bash
./main
```
## toiminta
Perustuu neljään tarkastukseen. 1. onko alue liikkunut viime framesta 2. onko muoto oikea 3. onko muoto oikean kokoinen 4 onko läpyskä vihreällä taustalla(maassa) eikä esim kädessä

Ohjelmaa voisi nopeuttaa jos muokkaisi video kokoa tai ei analyosisi jokaista framea.