g++ -Wall -std=c++14 -fPIC -O2 -c ./HeliosDac.cpp
g++ -Wall -std=c++14 -fPIC -O2 -c ./BEX.cpp
g++ -Wall -std=c++14 -fPIC -O2 -c ./DummyAdapter.cpp
g++ -Wall -std=c++14 -fPIC -O2 -c ./HeliosAdapter.cpp
g++ -Wall -std=c++14 -fPIC -O2 -c ./HWBridge.cpp
g++ -Wall -std=c++14 -fPIC -O2 -c ./IDNNode.cpp
g++ -Wall -std=c++14 -fPIC -O2 -c ./SPIDevAdapter.cpp
g++ -Wall -std=c++14 -fPIC -O2 -c ./IDNMain.cpp
g++ -o openidn BEX.o DummyAdapter.o HeliosAdapter.o HeliosDac.o HWBridge.o IDNNode.o SPIDevAdapter.o IDNMain.o -lusb-1.0 -lpthread